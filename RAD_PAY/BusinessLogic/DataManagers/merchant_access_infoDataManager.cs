using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_access_infoDataManager
    {
        //merchant_access_infoViewModel
        //public int        merchant_id { get; set; }
        //public string     login       { get; set; }
        //public string     password    { get; set; }
        //public string     api_json    { get; set; }
        //public string     url         { get; set; }

        public static void Add(merchant_access_infoViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new merchant_access_info
            {
                merchant_id = model.merchant_id, 
                login       = model.login      , 
                password    = model.password   , 
                api_json    = model.api_json   , 
                url         = model.url        ,
            };

            db.merchant_access_info.Add(dbmodel);

        }

        public static void Modify(merchant_access_infoViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_access_info.Where(z => z.merchant_id == model.merchant_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.merchant_id = model.merchant_id ; 
                    dbmodel.login = model.login             ; 
                    dbmodel.password = model.password       ; 
                    dbmodel.api_json = model.api_json       ;
                    dbmodel.url = model.url;
                }
            }
        }

        public static void Delete(merchant_access_infoViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_access_info.Where(z => z.merchant_id == model.merchant_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.merchant_access_info.Remove(dbmodel);
                }
            }
        }

        public static List<merchant_access_infoViewModel> Get(merchant_access_infoViewModel model, RAD_PAYEntities db)
        {
            List<merchant_access_infoViewModel> list = null;

            var query = from resmodel in db.merchant_access_info
                        select new merchant_access_infoViewModel
                        {
                            merchant_id = resmodel.merchant_id,
                            login = resmodel.login,
                            password = resmodel.password,
                            api_json = resmodel.api_json,
                            url = resmodel.url,
                        };

            list = query.ToList();

            return list;
        }
    }
}