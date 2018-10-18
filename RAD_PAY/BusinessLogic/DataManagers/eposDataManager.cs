using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class eposDataManager
    {
        //eposViewModel
        //public long       id          { get; set; }
        //public string     name        { get; set; }
        //public string     merchant_id { get; set; }
        //public string     terminal_id { get; set; }
        //public int?       port        { get; set; }
        //public int?       bank_id     { get; set; }
        //public int?       a           { get; set; }
        //public int?       b           { get; set; }
        //public int?       c           { get; set; }
        //public int?       d           { get; set; }

        public static void Add(eposViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new epos
            {
                    id          = model.id          ,
                    name        = model.name        ,
                    merchant_id = model.merchant_id ,
                    terminal_id = model.terminal_id ,
                    port        = model.port        ,
                    bank_id     = model.bank_id     ,
                    a           = model.a           ,
                    b           = model.b           ,
                    c           = model.c           ,
                    d           = model.d           ,
            };

            db.epos.Add(dbmodel);
        }

        public static void Modify(eposViewModel model, RAD_PAYEntities db)
        {
            var result = db.epos.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                       ;
                    dbmodel.name = model.name                   ;
                    dbmodel.merchant_id = model.merchant_id     ;
                    dbmodel.terminal_id = model.terminal_id     ;
                    dbmodel.port = model.port                   ;
                    dbmodel.bank_id = model.bank_id             ;
                    dbmodel.a = model.a                         ;
                    dbmodel.b = model.b                         ;
                    dbmodel.c = model.c                         ;
                    dbmodel.d = model.d;
                }
            }
        }

        public static void Delete(eposViewModel model, RAD_PAYEntities db)
        {
            var result = db.epos.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.epos.Remove(dbmodel);
                }
            }
        }

        public static List<eposViewModel> Get(eposViewModel model, RAD_PAYEntities db)
        {
            List<eposViewModel> list = null;

            var query = from resmodel in db.epos
                        select new eposViewModel
                        {
                            id = resmodel.id,
                            name = resmodel.name,
                            merchant_id = resmodel.merchant_id,
                            terminal_id = resmodel.terminal_id,
                            port = resmodel.port,
                            bank_id = resmodel.bank_id,
                            a = resmodel.a,
                            b = resmodel.b,
                            c = resmodel.c,
                            d = resmodel.d,
                        };

            list = query.ToList();

            return list;
        }
    }
}