using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_fields_dataDataManager
    {
        //merchant_fields_dataViewModel
        //public int        id                  { get; set; }
        //public int?       fid                 { get; set; }
        //public int?       key                 { get; set; }
        //public string     value               { get; set; }
        //public string     prefix              { get; set; }
        //public int?       extra_id            { get; set; }
        //public int        parent_key          { get; set; }
        //public int        service_id          { get; set; }
        //public int        service_id_check    { get; set; }

        public static void Add(merchant_fields_dataViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new merchant_fields_data
            {
                id               = model.id                 ,         
                fid              = model.fid                ,
                key              = model.key                ,
                value            = model.value              ,
                prefix           = model.prefix             ,
                extra_id         = model.extra_id           ,
                parent_key       = model.parent_key         ,
                service_id       = model.service_id         ,
                service_id_check = model.service_id_check   ,
            };

            db.merchant_fields_data.Add(dbmodel);
        }

        public static void Modify(merchant_fields_dataViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_fields_data.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                 ;         
                    dbmodel.fid = model.fid                ;
                    dbmodel.key = model.key                ;
                    dbmodel.value = model.value              ;
                    dbmodel.prefix = model.prefix             ;
                    dbmodel.extra_id = model.extra_id           ;
                    dbmodel.parent_key = model.parent_key         ;
                    dbmodel.service_id = model.service_id         ;
                    dbmodel.service_id_check = model.service_id_check   ;
                }
            }
        }

        public static void Delete(merchant_fields_dataViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_fields_data.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.merchant_fields_data.Remove(dbmodel);
                }
            }
        }

        public static List<merchant_fields_dataViewModel> Get(merchant_fields_dataViewModel model, RAD_PAYEntities db)
        {
            List<merchant_fields_dataViewModel> list = null;

            var query = from resmodel in db.merchant_fields_data
                        select new merchant_fields_dataViewModel
                        {
                            id = resmodel.id,
                            fid = resmodel.fid,
                            key = resmodel.key,
                            value = resmodel.value,
                            prefix = resmodel.prefix,
                            extra_id = resmodel.extra_id,
                            parent_key = resmodel.parent_key,
                            service_id = resmodel.service_id,
                            service_id_check = resmodel.service_id_check,
                        };

            list = query.ToList();

            return list;
        }
    }
}